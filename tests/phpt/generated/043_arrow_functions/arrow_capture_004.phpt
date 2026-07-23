--TEST--
arrow function implicit capture vector 004
--FILE--
<?php
$base = 17;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(6), ':', $add(7);
--EXPECT--
23:24
