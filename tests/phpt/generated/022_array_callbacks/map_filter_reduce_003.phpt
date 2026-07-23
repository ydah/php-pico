--TEST--
array callbacks vector 003
--FILE--
<?php
$values = [4, 5, 6, 7, 8]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
20,25,30,35,40:20,25,35,40:120
