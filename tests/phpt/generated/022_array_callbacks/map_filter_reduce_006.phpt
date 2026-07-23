--TEST--
array callbacks vector 006
--FILE--
<?php
$values = [7, 8, 9, 10, 11]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
28,32,36,40,44:28,32,40,44:144
