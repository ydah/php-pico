--TEST--
arrow function implicit capture vector 016
--FILE--
<?php
$base = 65;
$add = fn($value) => $base + $value;
$base = 9999;
echo $add(3), ':', $add(5);
--EXPECT--
68:70
