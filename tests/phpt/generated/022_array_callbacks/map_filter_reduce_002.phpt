--TEST--
array callbacks vector 002
--FILE--
<?php
$values = [3, 4, 5, 6, 7]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
12,16,20,24,28:16,20,28:64
