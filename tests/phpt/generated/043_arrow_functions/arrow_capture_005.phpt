--TEST--
arrow function implicit capture vector 005
--FILE--
<?php
$base = 21;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(2), ':', $add(8);
--EXPECT--
23:29
