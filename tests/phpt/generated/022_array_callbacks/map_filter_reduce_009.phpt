--TEST--
array callbacks vector 009
--FILE--
<?php
$values = [10, 11, 12, 13, 14]; $factor = 3;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
30,33,36,39,42::0
