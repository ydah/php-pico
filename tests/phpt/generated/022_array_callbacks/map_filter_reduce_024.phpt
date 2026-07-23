--TEST--
array callbacks vector 024
--FILE--
<?php
$values = [25, 26, 27, 28, 29]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
50,52,54,56,58:50,52,56,58:216
