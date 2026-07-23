--TEST--
array callbacks vector 007
--FILE--
<?php
$values = [8, 9, 10, 11, 12]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
40,45,50,55,60:40,50,55:145
