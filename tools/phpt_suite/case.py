"""PHPT case model and rendering."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Case:
    family: str
    slug: str
    title: str
    source: str
    expected: str

    @property
    def relative_path(self) -> str:
        return f"{self.family}/{self.slug}.phpt"

    def render(self) -> str:
        return (
            f"--TEST--\n{self.title}\n"
            f"--FILE--\n<?php\n{self.source.rstrip()}\n"
            f"--EXPECT--\n{self.expected.rstrip()}\n"
        )
