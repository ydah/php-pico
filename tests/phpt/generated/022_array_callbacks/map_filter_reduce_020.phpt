--TEST--
array callbacks vector 020
--FILE--
<?php
$values = [21, 22, 23, 24, 25]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
42,44,46,48,50:44,46,50:140
