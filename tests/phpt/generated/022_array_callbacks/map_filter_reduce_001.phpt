--TEST--
array callbacks vector 001
--FILE--
<?php
$values = [2, 3, 4, 5, 6]; $factor = 3;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
6,9,12,15,18::0
