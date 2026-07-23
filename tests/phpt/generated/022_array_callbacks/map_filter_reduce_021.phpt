--TEST--
array callbacks vector 021
--FILE--
<?php
$values = [22, 23, 24, 25, 26]; $factor = 3;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
66,69,72,75,78::0
