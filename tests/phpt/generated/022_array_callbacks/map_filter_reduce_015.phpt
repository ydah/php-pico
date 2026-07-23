--TEST--
array callbacks vector 015
--FILE--
<?php
$values = [16, 17, 18, 19, 20]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
80,85,90,95,100:80,85,95,100:360
