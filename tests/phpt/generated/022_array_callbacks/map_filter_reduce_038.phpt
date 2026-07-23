--TEST--
array callbacks vector 038
--FILE--
<?php
$values = [39, 40, 41, 42, 43]; $factor = 4;
$mapped = array_map(fn($value) => $value * $factor, $values);
$filtered = array_filter($mapped, fn($value) => $value % 3 !== 0);
$total = array_reduce($filtered, fn($carry, $value) => $carry + $value, 0);
echo implode(',', $mapped), ':', implode(',', $filtered), ':', $total;
--EXPECT--
156,160,164,168,172:160,164,172:496
