--TEST--
array callbacks vector 028
--FILE--
<?php
$values = [29, 30, 31, 32, 33]; $factor = 2;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
58,60,62,64,66:58,62,64:184
