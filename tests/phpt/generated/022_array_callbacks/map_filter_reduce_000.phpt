--TEST--
array callbacks vector 000
--FILE--
<?php
$values = [1, 2, 3, 4, 5]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
2,4,6,8,10:2,4,8,10:24
