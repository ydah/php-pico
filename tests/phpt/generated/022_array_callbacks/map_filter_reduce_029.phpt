--TEST--
array callbacks vector 029
--FILE--
<?php
$values = [30, 31, 32, 33, 34]; $factor = 3;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
90,93,96,99,102::0
