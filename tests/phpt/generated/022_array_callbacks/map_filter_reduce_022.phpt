--TEST--
array callbacks vector 022
--FILE--
<?php
$values = [23, 24, 25, 26, 27]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
92,96,100,104,108:92,100,104:296
