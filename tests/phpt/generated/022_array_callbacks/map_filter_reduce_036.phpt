--TEST--
array callbacks vector 036
--FILE--
<?php
$values = [37, 38, 39, 40, 41]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
74,76,78,80,82:74,76,80,82:312
