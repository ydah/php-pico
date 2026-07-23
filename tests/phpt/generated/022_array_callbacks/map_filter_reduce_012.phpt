--TEST--
array callbacks vector 012
--FILE--
<?php
$values = [13, 14, 15, 16, 17]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
26,28,30,32,34:26,28,32,34:120
