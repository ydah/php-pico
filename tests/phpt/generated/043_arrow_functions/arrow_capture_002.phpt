--TEST--
arrow function implicit capture vector 002
--FILE--
<?php
$base = 9;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(4), ':', $add(5);
--EXPECT--
13:14
