--TEST--
array callbacks vector 008
--FILE--
<?php
$values = [9, 10, 11, 12, 13]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
18,20,22,24,26:20,22,26:68
