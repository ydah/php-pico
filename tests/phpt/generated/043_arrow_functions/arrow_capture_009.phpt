--TEST--
arrow function implicit capture vector 009
--FILE--
<?php
$base = 37;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(6), ':', $add(5);
--EXPECT--
43:42
