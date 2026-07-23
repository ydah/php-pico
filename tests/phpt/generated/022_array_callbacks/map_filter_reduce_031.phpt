--TEST--
array callbacks vector 031
--FILE--
<?php
$values = [32, 33, 34, 35, 36]; $factor = 5;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
160,165,170,175,180:160,170,175:505
