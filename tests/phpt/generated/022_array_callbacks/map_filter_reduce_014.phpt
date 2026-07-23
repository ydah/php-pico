--TEST--
array callbacks vector 014
--FILE--
<?php
$values = [15, 16, 17, 18, 19]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
60,64,68,72,76:64,68,76:208
