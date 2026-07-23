--TEST--
array callbacks vector 019
--FILE--
<?php
$values = [20, 21, 22, 23, 24]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
100,105,110,115,120:100,110,115:325
