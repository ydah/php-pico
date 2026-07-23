--TEST--
array callbacks vector 023
--FILE--
<?php
$values = [24, 25, 26, 27, 28]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
120,125,130,135,140:125,130,140:395
