--TEST--
integer arithmetic vector 083
--FILE--
<?php
$a = 85; $b = 17;
echo $a + $b, ':', $a - $b, ':', $a * $b, ':', $a % $b, ':', ($a + $b) * 2 - $b, ':', $a << 3;
--EXPECT--
102:68:1445:0:187:680
