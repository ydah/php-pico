--TEST--
integer arithmetic vector 065
--FILE--
<?php
$a = 161; $b = 24;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 1;
--EXPECT--
185:137:3864:17:346:322
