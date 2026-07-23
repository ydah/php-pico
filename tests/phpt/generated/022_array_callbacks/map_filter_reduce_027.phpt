--TEST--
array callbacks vector 027
--FILE--
<?php
$values = [28, 29, 30, 31, 32]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
140,145,150,155,160:140,145,155,160:600
