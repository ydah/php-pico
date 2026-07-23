--TEST--
array callbacks vector 011
--FILE--
<?php
$values = [12, 13, 14, 15, 16]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
60,65,70,75,80:65,70,80:215
