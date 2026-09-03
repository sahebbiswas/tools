#!/usr/bin/env python3
"""Analyze Boolean conditions in C/C++ preprocessor conditional blocks.

This intentionally does not preprocess or parse C/C++ source code. It only
recognizes conditional directives and treats identifiers as Boolean flags.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Iterator, Optional, Sequence, Union


@dataclass(frozen=True)
class Constant:
    value: bool


@dataclass(frozen=True)
class Variable:
    name: str


@dataclass(frozen=True)
class Negation:
    operand: "Expression"


@dataclass(frozen=True)
class Conjunction:
    operands: tuple["Expression", ...]


@dataclass(frozen=True)
class Disjunction:
    operands: tuple["Expression", ...]


Expression = Union[Constant, Variable, Negation, Conjunction, Disjunction]
TRUE = Constant(True)
FALSE = Constant(False)


class ConditionError(ValueError):
    """Base class for input errors reported by the analyzer."""


class ExpressionSyntaxError(ConditionError):
    """Raised for an unsupported or malformed Boolean expression."""


class DirectiveStructureError(ConditionError):
    """Raised for unmatched or misplaced conditional directives."""


_TOKEN_RE = re.compile(
    r"\s*(?:"
    r"(?P<and>&&)|(?P<or>\|\|)|(?P<not>!)|"
    r"(?P<lparen>\()|(?P<rparen>\))|"
    r"(?P<number>0[xX][0-9a-fA-F]+[uUlL]*|[0-9]+[uUlL]*)|"
    r"(?P<identifier>[A-Za-z_]\w*)|(?P<invalid>\S)"
    r")"
)


@dataclass(frozen=True)
class _Token:
    kind: str
    text: str
    offset: int


def _tokens(text: str) -> list[_Token]:
    result: list[_Token] = []
    offset = 0
    while offset < len(text):
        match = _TOKEN_RE.match(text, offset)
        if not match:
            if text[offset:].strip():
                raise ExpressionSyntaxError(
                    f"unsupported input at column {offset + 1}: {text[offset:]!r}"
                )
            break
        kind = match.lastgroup
        assert kind is not None
        token_text = match.group(kind)
        if kind == "invalid":
            raise ExpressionSyntaxError(
                f"unsupported token {token_text!r} at column {match.start(kind) + 1}"
            )
        result.append(_Token(kind, token_text, match.start(kind)))
        offset = match.end()
    return result


class _ExpressionParser:
    def __init__(self, text: str):
        self.text = text
        self.tokens = _tokens(text)
        self.position = 0

    def parse(self) -> Expression:
        if not self.tokens:
            raise ExpressionSyntaxError("expected a Boolean expression")
        expression = self._parse_or()
        if self.position != len(self.tokens):
            token = self.tokens[self.position]
            raise ExpressionSyntaxError(
                f"unexpected token {token.text!r} at column {token.offset + 1}"
            )
        return expression

    def _accept(self, kind: str) -> Optional[_Token]:
        if self.position < len(self.tokens) and self.tokens[self.position].kind == kind:
            token = self.tokens[self.position]
            self.position += 1
            return token
        return None

    def _expect(self, kind: str, description: str) -> _Token:
        token = self._accept(kind)
        if token is None:
            column = (
                self.tokens[self.position].offset + 1
                if self.position < len(self.tokens)
                else len(self.text) + 1
            )
            raise ExpressionSyntaxError(f"expected {description} at column {column}")
        return token

    def _parse_or(self) -> Expression:
        operands = [self._parse_and()]
        while self._accept("or"):
            operands.append(self._parse_and())
        return operands[0] if len(operands) == 1 else Disjunction(tuple(operands))

    def _parse_and(self) -> Expression:
        operands = [self._parse_unary()]
        while self._accept("and"):
            operands.append(self._parse_unary())
        return operands[0] if len(operands) == 1 else Conjunction(tuple(operands))

    def _parse_unary(self) -> Expression:
        if self._accept("not"):
            return Negation(self._parse_unary())
        return self._parse_primary()

    def _parse_primary(self) -> Expression:
        if self._accept("lparen"):
            expression = self._parse_or()
            self._expect("rparen", "')'")
            return expression

        token = self._accept("number")
        if token:
            digits = re.sub(r"[uUlL]+$", "", token.text)
            if digits.lower().startswith("0x"):
                base = 16
            elif len(digits) > 1 and digits.startswith("0"):
                base = 8
            else:
                base = 10
            try:
                return Constant(int(digits, base) != 0)
            except ValueError as error:
                raise ExpressionSyntaxError(
                    f"invalid integer {token.text!r} at column {token.offset + 1}"
                ) from error

        token = self._accept("identifier")
        if token is None:
            column = (
                self.tokens[self.position].offset + 1
                if self.position < len(self.tokens)
                else len(self.text) + 1
            )
            raise ExpressionSyntaxError(f"expected a flag or '(' at column {column}")

        if token.text != "defined":
            return Variable(token.text)
        if self._accept("lparen"):
            name = self._expect("identifier", "a macro name")
            self._expect("rparen", "')'")
        else:
            name = self._expect("identifier", "a macro name")
        return Variable(name.text)


def parse_expression(text: str) -> Expression:
    """Parse the supported Boolean subset of a preprocessor expression."""

    return _ExpressionParser(text).parse()


def _sort_key(expression: Expression) -> str:
    return format_expression(expression)


def negate(expression: Expression) -> Expression:
    expression = simplify(expression)
    if isinstance(expression, Constant):
        return Constant(not expression.value)
    if isinstance(expression, Negation):
        return expression.operand
    return Negation(expression)


def conjunction(*expressions: Expression) -> Expression:
    operands: list[Expression] = []
    for expression in expressions:
        expression = simplify(expression)
        if expression == FALSE:
            return FALSE
        if expression == TRUE:
            continue
        if isinstance(expression, Conjunction):
            operands.extend(expression.operands)
        else:
            operands.append(expression)
    unique = set(operands)
    if any(negate(operand) in unique for operand in unique):
        return FALSE
    filtered = [
        operand
        for operand in unique
        if not (
            isinstance(operand, Disjunction)
            and any(term in unique for term in operand.operands)
        )
    ]
    if not filtered:
        return TRUE
    if len(filtered) == 1:
        return filtered[0]
    return Conjunction(tuple(sorted(filtered, key=_sort_key)))


def disjunction(*expressions: Expression) -> Expression:
    operands: list[Expression] = []
    for expression in expressions:
        expression = simplify(expression)
        if expression == TRUE:
            return TRUE
        if expression == FALSE:
            continue
        if isinstance(expression, Disjunction):
            operands.extend(expression.operands)
        else:
            operands.append(expression)
    unique = set(operands)
    if any(negate(operand) in unique for operand in unique):
        return TRUE
    filtered = [
        operand
        for operand in unique
        if not (
            isinstance(operand, Conjunction)
            and any(term in unique for term in operand.operands)
        )
    ]
    if not filtered:
        return FALSE
    if len(filtered) == 1:
        return filtered[0]
    return Disjunction(tuple(sorted(filtered, key=_sort_key)))


def simplify(expression: Expression) -> Expression:
    """Apply Boolean identities, including complements and absorption."""

    if isinstance(expression, (Constant, Variable)):
        return expression
    if isinstance(expression, Negation):
        return negate(expression.operand)
    if isinstance(expression, Conjunction):
        return conjunction(*expression.operands)
    return disjunction(*expression.operands)


def _precedence(expression: Expression) -> int:
    if isinstance(expression, Disjunction):
        return 1
    if isinstance(expression, Conjunction):
        return 2
    if isinstance(expression, Negation):
        return 3
    return 4


def format_expression(expression: Expression, parent_precedence: int = 0) -> str:
    """Format an expression using conventional preprocessor operators."""

    if isinstance(expression, Constant):
        text = "1" if expression.value else "0"
    elif isinstance(expression, Variable):
        text = expression.name
    elif isinstance(expression, Negation):
        text = f"!{format_expression(expression.operand, _precedence(expression))}"
    else:
        operator = " && " if isinstance(expression, Conjunction) else " || "
        precedence = _precedence(expression)
        text = operator.join(
            format_expression(item, precedence) for item in expression.operands
        )
    return f"({text})" if _precedence(expression) < parent_precedence else text


def expression_variables(expression: Expression) -> set[str]:
    if isinstance(expression, Constant):
        return set()
    if isinstance(expression, Variable):
        return {expression.name}
    if isinstance(expression, Negation):
        return expression_variables(expression.operand)
    result: set[str] = set()
    for operand in expression.operands:
        result.update(expression_variables(operand))
    return result


class _BDD:
    """Small dependency-free ROBDD engine used for exact logical queries."""

    def __init__(self, variables: Iterable[str]):
        names = sorted(set(variables))
        self.order = {name: index for index, name in enumerate(names)}
        self.names = names
        self.nodes: list[Optional[tuple[int, int, int]]] = [None, None]
        self.unique: dict[tuple[int, int, int], int] = {}
        self._apply_cache: dict[tuple[str, int, int], int] = {}
        self._not_cache: dict[int, int] = {0: 1, 1: 0}
        self._build_cache: dict[Expression, int] = {}
        self._expression_cache: dict[int, Expression] = {0: FALSE, 1: TRUE}

    def _node(self, variable: int, low: int, high: int) -> int:
        if low == high:
            return low
        key = (variable, low, high)
        if key not in self.unique:
            self.unique[key] = len(self.nodes)
            self.nodes.append(key)
        return self.unique[key]

    def build(self, expression: Expression) -> int:
        expression = simplify(expression)
        cached = self._build_cache.get(expression)
        if cached is not None:
            return cached
        if isinstance(expression, Constant):
            result = int(expression.value)
        elif isinstance(expression, Variable):
            result = self._node(self.order[expression.name], 0, 1)
        elif isinstance(expression, Negation):
            result = self.negate(self.build(expression.operand))
        elif isinstance(expression, Conjunction):
            result = 1
            for operand in expression.operands:
                result = self.apply("and", result, self.build(operand))
        else:
            result = 0
            for operand in expression.operands:
                result = self.apply("or", result, self.build(operand))
        self._build_cache[expression] = result
        return result

    def negate(self, node: int) -> int:
        if node in self._not_cache:
            return self._not_cache[node]
        item = self.nodes[node]
        assert item is not None
        variable, low, high = item
        result = self._node(variable, self.negate(low), self.negate(high))
        self._not_cache[node] = result
        return result

    def apply(self, operation: str, left: int, right: int) -> int:
        if left > right:
            left, right = right, left
        key = (operation, left, right)
        if key in self._apply_cache:
            return self._apply_cache[key]
        if operation == "and":
            if left == 0 or right == 0:
                return 0
            if left == 1:
                return right
            if left == right:
                return left
        elif operation == "or":
            if left == 1 or right == 1:
                return 1
            if left == 0:
                return right
            if left == right:
                return left
        else:
            raise ValueError(f"unknown BDD operation: {operation}")

        left_node = self.nodes[left]
        right_node = self.nodes[right]
        assert left_node is not None and right_node is not None
        variable = min(left_node[0], right_node[0])
        left_low, left_high = (
            (left_node[1], left_node[2]) if left_node[0] == variable else (left, left)
        )
        right_low, right_high = (
            (right_node[1], right_node[2])
            if right_node[0] == variable
            else (right, right)
        )
        result = self._node(
            variable,
            self.apply(operation, left_low, right_low),
            self.apply(operation, left_high, right_high),
        )
        self._apply_cache[key] = result
        return result

    def satisfiable(self, expression: Expression) -> bool:
        return self.build(expression) != 0

    def to_expression(self, node: int) -> Expression:
        """Convert a BDD to a reasonably compact equivalent expression."""

        cached = self._expression_cache.get(node)
        if cached is not None:
            return cached
        item = self.nodes[node]
        assert item is not None
        variable_index, low_node, high_node = item
        variable = Variable(self.names[variable_index])
        low = self.to_expression(low_node)
        high = self.to_expression(high_node)
        if low == FALSE:
            result = conjunction(variable, high)
        elif high == FALSE:
            result = conjunction(negate(variable), low)
        elif low == TRUE:
            result = disjunction(negate(variable), high)
        elif high == TRUE:
            result = disjunction(variable, low)
        else:
            result = disjunction(
                conjunction(negate(variable), low),
                conjunction(variable, high),
            )
        self._expression_cache[node] = result
        return result

    def equivalent_under(
        self, context: Expression, left: Expression, right: Expression
    ) -> bool:
        left_node = self.build(left)
        right_node = self.build(right)
        difference = self.apply(
            "or",
            self.apply("and", left_node, self.negate(right_node)),
            self.apply("and", self.negate(left_node), right_node),
        )
        return self.apply("and", self.build(context), difference) == 0


def _expression_size(expression: Expression) -> int:
    if isinstance(expression, (Constant, Variable)):
        return 1
    if isinstance(expression, Negation):
        return 1 + _expression_size(expression.operand)
    return 1 + sum(_expression_size(operand) for operand in expression.operands)


def exact_simplify(expression: Expression, bdd: _BDD) -> Expression:
    """Return the smaller of algebraic and canonical exact simplifications."""

    algebraic = simplify(expression)
    canonical = bdd.to_expression(bdd.build(algebraic))
    if _expression_size(canonical) < _expression_size(algebraic):
        return canonical
    return algebraic


def simplify_under(
    expression: Expression, context: Expression, bdd: _BDD
) -> Expression:
    """Simplify an expression using facts guaranteed by an enclosing context."""

    expression = simplify(expression)
    if bdd.equivalent_under(context, expression, TRUE):
        return TRUE
    if bdd.equivalent_under(context, expression, FALSE):
        return FALSE
    if isinstance(expression, Negation):
        return negate(simplify_under(expression.operand, context, bdd))
    if not isinstance(expression, (Conjunction, Disjunction)):
        return expression

    operands = [simplify_under(item, context, bdd) for item in expression.operands]
    constructor = conjunction if isinstance(expression, Conjunction) else disjunction
    candidate = constructor(*operands)
    if not isinstance(candidate, (Conjunction, Disjunction)):
        return candidate

    operands = list(candidate.operands)
    changed = True
    while changed and len(operands) > 1:
        changed = False
        for index in range(len(operands)):
            trial = constructor(*(operands[:index] + operands[index + 1 :]))
            if bdd.equivalent_under(context, candidate, trial):
                candidate = trial
                operands.pop(index)
                changed = True
                break
    return candidate


@dataclass
class ConditionalBranch:
    directive: str
    line: int
    expression_text: Optional[str]
    expression: Optional[Expression]
    children: list["ConditionalGroup"] = field(default_factory=list)
    analysis: Optional["BranchAnalysis"] = None


@dataclass
class ConditionalGroup:
    line: int
    end_line: Optional[int] = None
    branches: list[ConditionalBranch] = field(default_factory=list)


@dataclass
class ConditionalTree:
    groups: list[ConditionalGroup] = field(default_factory=list)


@dataclass(frozen=True)
class BranchAnalysis:
    status: str
    simplified: Optional[Expression]
    contextual: Optional[Expression]
    effective: Expression
    reason: Optional[str] = None


_DIRECTIVE_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)$")


def _strip_comments(source: str) -> str:
    """Remove comments while preserving newlines and therefore line numbers."""

    def block_replacement(match: re.Match[str]) -> str:
        return "\n" * match.group(0).count("\n")

    source = re.sub(r"/\*.*?\*/", block_replacement, source, flags=re.DOTALL)
    return re.sub(r"//[^\n]*", "", source)


def _logical_lines(source: str) -> Iterator[tuple[int, str]]:
    lines = _strip_comments(source).splitlines()
    index = 0
    while index < len(lines):
        start = index + 1
        text = lines[index]
        while text.rstrip().endswith("\\") and index + 1 < len(lines):
            text = text.rstrip()[:-1] + " " + lines[index + 1].lstrip()
            index += 1
        yield start, text
        index += 1


def _directive_expression(
    kind: str, remainder: str, line: int
) -> tuple[str, Expression]:
    text = remainder.strip()
    if kind in {"ifdef", "ifndef"}:
        if not re.fullmatch(r"[A-Za-z_]\w*", text):
            raise ExpressionSyntaxError(
                f"line {line}: #{kind} expects exactly one macro name"
            )
        expression: Expression = Variable(text)
        if kind == "ifndef":
            expression = Negation(expression)
        return text, expression
    try:
        return text, parse_expression(text)
    except ExpressionSyntaxError as error:
        raise ExpressionSyntaxError(f"line {line}: {error}") from error


def parse_source(source: str) -> ConditionalTree:
    """Build a tree containing only conditional preprocessor directives."""

    tree = ConditionalTree()
    stack: list[tuple[ConditionalGroup, ConditionalBranch]] = []

    for line, text in _logical_lines(source):
        match = _DIRECTIVE_RE.match(text)
        if not match:
            continue
        kind, remainder = match.group(1), match.group(2)

        if kind in {"if", "ifdef", "ifndef"}:
            expression_text, expression = _directive_expression(kind, remainder, line)
            group = ConditionalGroup(line)
            branch = ConditionalBranch(kind, line, expression_text, expression)
            group.branches.append(branch)
            if stack:
                stack[-1][1].children.append(group)
            else:
                tree.groups.append(group)
            stack.append((group, branch))
            continue

        if not stack:
            raise DirectiveStructureError(f"line {line}: #{kind} has no matching #if")
        group, current = stack[-1]

        if kind == "elif":
            if current.directive == "else":
                raise DirectiveStructureError(f"line {line}: #elif appears after #else")
            expression_text, expression = _directive_expression(kind, remainder, line)
            branch = ConditionalBranch(kind, line, expression_text, expression)
            group.branches.append(branch)
            stack[-1] = (group, branch)
        elif kind == "else":
            if remainder.strip():
                raise DirectiveStructureError(
                    f"line {line}: unexpected text after #else"
                )
            if current.directive == "else":
                raise DirectiveStructureError(f"line {line}: duplicate #else")
            branch = ConditionalBranch(kind, line, None, None)
            group.branches.append(branch)
            stack[-1] = (group, branch)
        else:
            if remainder.strip():
                raise DirectiveStructureError(
                    f"line {line}: unexpected text after #endif"
                )
            group.end_line = line
            stack.pop()

    if stack:
        group, _ = stack[-1]
        raise DirectiveStructureError(f"line {group.line}: #if has no matching #endif")
    return tree


def _tree_expressions(groups: Sequence[ConditionalGroup]) -> Iterator[Expression]:
    for group in groups:
        for branch in group.branches:
            if branch.expression is not None:
                yield branch.expression
            yield from _tree_expressions(branch.children)


def analyze_tree(tree: ConditionalTree) -> ConditionalTree:
    """Annotate each branch with reachability and simplification results."""

    variables: set[str] = set()
    for expression in _tree_expressions(tree.groups):
        variables.update(expression_variables(expression))
    bdd = _BDD(variables)

    def analyze_groups(groups: Sequence[ConditionalGroup], parent: Expression) -> None:
        for group in groups:
            covered: Expression = FALSE
            for branch in group.branches:
                available = conjunction(parent, negate(covered))
                condition = branch.expression if branch.expression is not None else TRUE
                effective = conjunction(available, condition)
                simplified = (
                    exact_simplify(condition, bdd)
                    if branch.expression is not None
                    else None
                )
                contextual = (
                    simplify_under(condition, available, bdd)
                    if branch.expression is not None and bdd.satisfiable(available)
                    else simplified
                )

                if not bdd.satisfiable(parent):
                    status = "dead"
                    reason = "enclosing branch is unreachable"
                elif not bdd.satisfiable(available):
                    status = "dead"
                    reason = "earlier branch conditions cover every remaining case"
                elif not bdd.satisfiable(effective):
                    status = "dead"
                    reason = "condition contradicts its parent or earlier branches"
                elif branch.expression is not None and contextual == TRUE:
                    status = "redundant"
                    reason = "condition is always true in this branch context"
                else:
                    status = "reachable"
                    reason = None

                branch.analysis = BranchAnalysis(
                    status=status,
                    simplified=simplified,
                    contextual=contextual,
                    effective=simplify(effective),
                    reason=reason,
                )
                analyze_groups(branch.children, effective)
                covered = (
                    TRUE
                    if branch.expression is None
                    else disjunction(covered, condition)
                )

    analyze_groups(tree.groups, TRUE)
    return tree


def analyze_source(source: str) -> ConditionalTree:
    """Parse and analyze conditional directives in source text."""

    return analyze_tree(parse_source(source))


def _branch_dict(branch: ConditionalBranch) -> dict[str, object]:
    assert branch.analysis is not None
    return {
        "directive": branch.directive,
        "line": branch.line,
        "condition": branch.expression_text,
        "status": branch.analysis.status,
        "simplified_condition": (
            format_expression(branch.analysis.simplified)
            if branch.analysis.simplified is not None
            else None
        ),
        "contextual_condition": (
            format_expression(branch.analysis.contextual)
            if branch.analysis.contextual is not None
            else None
        ),
        "effective_condition": format_expression(branch.analysis.effective),
        "reason": branch.analysis.reason,
        "children": [_group_dict(group) for group in branch.children],
    }


def _group_dict(group: ConditionalGroup) -> dict[str, object]:
    return {
        "line": group.line,
        "end_line": group.end_line,
        "branches": [_branch_dict(branch) for branch in group.branches],
    }


def tree_to_dict(tree: ConditionalTree) -> dict[str, object]:
    """Convert an analyzed tree to a JSON-serializable dictionary."""

    return {"groups": [_group_dict(group) for group in tree.groups]}


def _text_lines(groups: Sequence[ConditionalGroup], depth: int = 0) -> Iterator[str]:
    for group in groups:
        for branch in group.branches:
            assert branch.analysis is not None
            condition = f" {branch.expression_text}" if branch.expression_text else ""
            yield (
                f"{'  ' * depth}{branch.line}: #{branch.directive}{condition} "
                f"[{branch.analysis.status}]"
            )
            if branch.analysis.reason:
                yield f"{'  ' * (depth + 1)}reason: {branch.analysis.reason}"
            if branch.analysis.simplified is not None:
                simplified = format_expression(branch.analysis.simplified)
                contextual = format_expression(
                    branch.analysis.contextual or branch.analysis.simplified
                )
                yield f"{'  ' * (depth + 1)}simplified: {simplified}"
                if contextual != simplified:
                    yield f"{'  ' * (depth + 1)}in context: {contextual}"
            yield (
                f"{'  ' * (depth + 1)}effective: "
                f"{format_expression(branch.analysis.effective)}"
            )
            yield from _text_lines(branch.children, depth + 1)


def format_report(tree: ConditionalTree) -> str:
    """Render a compact, indented representation of an analyzed tree."""

    return "\n".join(_text_lines(tree.groups)) or "No conditional directives found."


def _has_findings(tree: ConditionalTree) -> bool:
    for group in tree.groups:
        for branch in group.branches:
            assert branch.analysis is not None
            if branch.analysis.status in {"dead", "redundant"}:
                return True
            if _has_findings(ConditionalTree(branch.children)):
                return True
    return False


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(
        description="Analyze Boolean C/C++ preprocessor conditional directives."
    )
    parser.add_argument("source", type=Path, help="C/C++-style source file to analyze")
    parser.add_argument(
        "--json", action="store_true", help="write the conditional tree as JSON"
    )
    parser.add_argument(
        "--fail-on-findings",
        action="store_true",
        help="exit with status 1 when a dead or redundant branch is found",
    )
    args = parser.parse_args(argv)

    try:
        source = args.source.read_text(encoding="utf-8")
        tree = analyze_source(source)
    except OSError as error:
        parser.error(str(error))
    except (ConditionError, UnicodeDecodeError) as error:
        print(f"{args.source}: {error}", file=sys.stderr)
        return 2

    if args.json:
        print(json.dumps(tree_to_dict(tree), indent=2))
    else:
        print(format_report(tree))
    return 1 if args.fail_on_findings and _has_findings(tree) else 0


if __name__ == "__main__":
    raise SystemExit(main())
