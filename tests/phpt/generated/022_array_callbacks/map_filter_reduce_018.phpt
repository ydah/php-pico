--TEST--
array callbacks vector 018
--FILE--
<?php
$values = [19, 20, 21, 22, 23]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
76,80,84,88,92:76,80,88,92:336
