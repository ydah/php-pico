--TEST--
array callbacks vector 010
--FILE--
<?php
$values = [11, 12, 13, 14, 15]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
44,48,52,56,60:44,52,56:152
