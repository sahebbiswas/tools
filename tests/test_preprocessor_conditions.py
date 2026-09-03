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
    tree = conditions.analyze_source(
        "#if (A && B) || (A && !B)\n#endif\n"
    )

    assert conditions.format_expression(branch(tree).analysis.simplified) == "A"


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


@pytest.mark.parametrize(
    "source,message",
    [
        ("#elif A\n", "no matching #if"),
        ("#if A\n#else\n#else\n#endif\n", "duplicate #else"),
        ("#if A\n", "no matching #endif"),
        ("#if A == B\n#endif\n", "unsupported token"),
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
