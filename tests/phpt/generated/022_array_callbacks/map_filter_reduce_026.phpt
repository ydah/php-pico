--TEST--
array callbacks vector 026
--FILE--
<?php
$values = [27, 28, 29, 30, 31]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
108,112,116,120,124:112,116,124:352
