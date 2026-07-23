--TEST--
array callbacks vector 004
--FILE--
<?php
$values = [5, 6, 7, 8, 9]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
10,12,14,16,18:10,14,16:40
