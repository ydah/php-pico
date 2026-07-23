--TEST--
array callbacks vector 032
--FILE--
<?php
$values = [33, 34, 35, 36, 37]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
66,68,70,72,74:68,70,74:212
