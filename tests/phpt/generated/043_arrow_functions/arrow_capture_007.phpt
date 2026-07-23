--TEST--
arrow function implicit capture vector 007
--FILE--
<?php
$base = 29;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(4), ':', $add(3);
--EXPECT--
33:32
