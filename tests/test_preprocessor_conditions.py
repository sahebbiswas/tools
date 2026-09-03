import json
import subprocess
import sys
from pathlib import Path

import pytest

SCRIPT = Path(__file__).parents[1] / "preprocessor_conditions.py"
sys.path.insert(0, str(SCRIPT.parent))

import preprocessor_conditions as conditions  # noqa: E402


def branch(tree, group=0, index=0):
    return tree.groups[group].branches[index]


def test_requested_absorption_example_simplifies_to_a():
    expression = conditions.parse_expression("(A && B && (C || !D)) || A")

    assert conditions.format_expression(conditions.simplify(expression)) == "A"


def test_complements_and_constants_are_simplified():
    expression = conditions.parse_expression("(A && !A) || (B && 1) || 0")

    assert conditions.format_expression(conditions.simplify(expression)) == "B"


def test_exact_reported_simplification_handles_consensus_identity():
    tree = conditions.analyze_source("#if (A && B) || (A && !B)\n#endif\n")

    assert conditions.format_expression(branch(tree).analysis.simplified) == "A"


def test_bdd_variable_order_follows_first_appearance():
    expression = conditions.parse_expression("Z && A && Z && M")
    atoms = conditions._expression_atoms_in_order(expression)

    bdd = conditions._BDD(atoms)

    assert bdd.atoms == [
        conditions.Variable("Z"),
        conditions.Variable("A"),
        conditions.Variable("M"),
    ]


def test_first_appearance_order_limits_synthetic_bdd_size():
    # Pairwise equivalence has a linear ROBDD when each pair is adjacent, but
    # the former alphabetical order grouped all uppercase and lowercase flags
    # and produced 3,069 reachable nodes for ten pairs.
    clauses = [
        f"(({upper} && {lower}) || (!{upper} && !{lower}))"
        for upper, lower in zip("ABCDEFGHIJ", "abcdefghij")
    ]
    expression = conditions.parse_expression(" && ".join(clauses))
    bdd = conditions._BDD(conditions._expression_atoms_in_order(expression))

    root = bdd.build(expression)

    assert bdd.node_count(root) == 30


def test_elif_shadowed_by_broader_if_is_dead():
    tree = conditions.analyze_source(
        """
#if (A || B)
ignored();
#elif A
also_ignored();
#endif
"""
    )

    assert branch(tree, index=0).analysis.status == "reachable"
    assert branch(tree, index=1).analysis.status == "dead"
    assert "contradicts" in branch(tree, index=1).analysis.reason


def test_nested_repeated_condition_is_redundant():
    tree = conditions.analyze_source(
        """
#if A
#if A || B
#endif
#endif
"""
    )

    nested = branch(tree).children[0].branches[0]
    assert nested.analysis.status == "redundant"
    assert conditions.format_expression(nested.analysis.contextual) == "1"


def test_nested_condition_is_decayed_using_parent_context():
    tree = conditions.analyze_source(
        """
#if A
#if A && (B || !A)
#endif
#endif
"""
    )

    nested = branch(tree).children[0].branches[0]
    assert nested.analysis.status == "reachable"
    assert conditions.format_expression(nested.analysis.contextual) == "B"


def test_else_is_dead_after_exhaustive_conditions():
    tree = conditions.analyze_source(
        """
#if A
#elif !A
#else
#endif
"""
    )

    assert branch(tree, index=2).analysis.status == "dead"
    assert "cover every remaining case" in branch(tree, index=2).analysis.reason


def test_dead_parent_makes_nested_branch_dead():
    tree = conditions.analyze_source(
        """
#if 0
#if A
#endif
#endif
"""
    )

    nested = branch(tree).children[0].branches[0]
    assert nested.analysis.status == "dead"
    assert nested.analysis.reason == "enclosing branch is unreachable"


def test_ifdef_ifndef_and_defined_forms():
    tree = conditions.analyze_source(
        """
#ifdef FEATURE
#elif defined(FALLBACK) && !defined DISABLED
#endif
#ifndef OTHER
#endif
"""
    )

    assert conditions.format_expression(branch(tree).expression) == "FEATURE"
    assert (
        conditions.format_expression(branch(tree, index=1).expression)
        == "FALLBACK && !DISABLED"
    )
    assert conditions.format_expression(branch(tree, group=1).expression) == "!OTHER"


def test_value_expression_becomes_opaque_predicate_without_losing_boolean_shape():
    expression = conditions.parse_expression("VERSION >= 4 && defined(FOO)")

    assert isinstance(expression, conditions.Conjunction)
    assert conditions.Predicate("VERSION >= 4") in expression.operands
    assert conditions.Variable("FOO") in expression.operands


def test_parent_flag_reasoning_continues_with_opaque_predicate():
    tree = conditions.analyze_source(
        """
#if VERSION >= 4 && defined(FOO)
#if defined(FOO)
#endif
#endif
"""
    )

    outer = branch(tree)
    nested = outer.children[0].branches[0]
    assert nested.analysis.status == "redundant"
    assert conditions.expression_predicates(outer.expression) == {"VERSION >= 4"}


def test_equivalent_repeated_opaque_predicate_makes_elif_dead():
    tree = conditions.analyze_source(
        """
#if VERSION >= 4 && FOO
#elif FOO && VERSION >= 4
#endif
"""
    )

    assert branch(tree, index=1).analysis.status == "dead"


def test_comparison_not_equal_is_not_parsed_as_boolean_negation():
    expression = conditions.parse_expression("VERSION != 4 && !DISABLED")

    assert isinstance(expression, conditions.Conjunction)
    assert conditions.Predicate("VERSION != 4") in expression.operands
    assert conditions.Negation(conditions.Variable("DISABLED")) in expression.operands


def test_boolean_negation_of_parenthesized_predicate_is_preserved():
    expression = conditions.parse_expression("!(VERSION >= 4)")

    assert expression == conditions.Negation(conditions.Predicate("VERSION >= 4"))
    assert conditions.format_expression(expression) == "!(VERSION >= 4)"


def test_unparenthesized_value_negation_remains_inside_opaque_predicate():
    expression = conditions.parse_expression("!VERSION >= 4")

    assert expression == conditions.Predicate("! VERSION >= 4")


def test_logical_text_inside_function_argument_remains_opaque():
    expression = conditions.parse_expression(
        '__has_include("platform( && )config.h") && FEATURE'
    )

    assert isinstance(expression, conditions.Conjunction)
    assert (
        conditions.Predicate('__has_include("platform( && )config.h")')
        in expression.operands
    )


def test_multiline_directive_and_comments_preserve_start_line():
    tree = conditions.analyze_source(
        """// heading
#if A && \\
    (B || B) /* duplicate */
#endif
"""
    )

    assert branch(tree).line == 2
    assert conditions.format_expression(branch(tree).analysis.simplified) == "A && B"


def test_multiline_directive_error_reports_physical_line_and_column():
    source = "#if A && \\\n    B)\n#endif\n"

    with pytest.raises(
        conditions.ExpressionSyntaxError,
        match=r"unexpected '\)' at line 2, column 6",
    ) as error:
        conditions.analyze_source(source)

    assert error.value.location == conditions._SourceLocation(line=2, column=6)


def test_locationless_expression_error_gets_directive_line_prefix():
    with pytest.raises(conditions.ExpressionSyntaxError) as error:
        conditions.analyze_source("#if\n#endif\n")

    assert error.value.location is None
    assert str(error.value) == "line 1: expected a Boolean expression"


@pytest.mark.parametrize(
    "source,message",
    [
        ("#elif A\n", "no matching #if"),
        ("#if A\n#else\n#else\n#endif\n", "duplicate #else"),
        ("#if A\n", "no matching #endif"),
        ("#if A &&\n#endif\n", "expected an operand"),
        ("#if (A || B\n#endif\n", r"expected.*'\)'"),
        ("#if 08\n#endif\n", "invalid integer"),
    ],
)
def test_malformed_input_has_a_clear_diagnostic(source, message):
    with pytest.raises(conditions.ConditionError, match=message):
        conditions.analyze_source(source)


def test_json_tree_preserves_nesting():
    tree = conditions.analyze_source("#if A\n#if B\n#endif\n#endif\n")

    result = conditions.tree_to_dict(tree)
    nested = result["groups"][0]["branches"][0]["children"][0]
    assert result["groups"][0]["end_line"] == 4
    assert nested["end_line"] == 3
    assert nested["branches"][0]["condition"] == "B"


def test_json_identifies_opaque_predicates():
    tree = conditions.analyze_source("#if (X + 1) >= LIMIT\n#endif\n")

    result = conditions.tree_to_dict(tree)
    assert result["groups"][0]["branches"][0]["opaque_predicates"] == [
        "(X + 1) >= LIMIT"
    ]


def test_text_report_labels_opaque_predicates():
    tree = conditions.analyze_source("#if VERSION >= 4 && FOO\n#endif\n")

    assert "opaque: VERSION >= 4" in conditions.format_report(tree)


def test_cli_json_and_fail_on_findings(tmp_path):
    source = tmp_path / "sample.c"
    source.write_text("#if A || B\n#elif A\n#endif\n", encoding="utf-8")

    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(source), "--json", "--fail-on-findings"],
        capture_output=True,
        check=False,
        text=True,
    )

    assert result.returncode == 1
    assert json.loads(result.stdout)["groups"][0]["branches"][1]["status"] == "dead"


def test_cli_reports_non_utf8_input_without_traceback(tmp_path):
    source = tmp_path / "invalid.c"
    source.write_bytes(b"#if A\n\xff\n#endif\n")

    result = subprocess.run(
        [sys.executable, str(SCRIPT), str(source)],
        capture_output=True,
        check=False,
        text=True,
    )

    assert result.returncode == 2
    assert str(source) in result.stderr
    assert "codec can't decode byte" in result.stderr
    assert "Traceback" not in result.stderr
