--TEST--
arrow function implicit capture vector 010
--FILE--
<?php
$base = 41;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(2), ':', $add(6);
--EXPECT--
43:47
