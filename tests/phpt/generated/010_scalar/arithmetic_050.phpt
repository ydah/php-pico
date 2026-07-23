--TEST--
integer arithmetic vector 050
--FILE--
<?php
$a = 97; $b = 3;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 2;
--EXPECT--
100:94:291:1:197:388
