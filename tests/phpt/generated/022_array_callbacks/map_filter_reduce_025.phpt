--TEST--
array callbacks vector 025
--FILE--
<?php
$values = [26, 27, 28, 29, 30]; $factor = 3;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
78,81,84,87,90::0
