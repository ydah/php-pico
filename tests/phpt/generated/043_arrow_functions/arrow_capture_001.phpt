--TEST--
arrow function implicit capture vector 001
--FILE--
<?php
$base = 5;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(3), ':', $add(4);
--EXPECT--
8:9
