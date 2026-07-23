--TEST--
array callbacks vector 035
--FILE--
<?php
$values = [36, 37, 38, 39, 40]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
180,185,190,195,200:185,190,200:575
