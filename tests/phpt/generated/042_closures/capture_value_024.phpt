--TEST--
closure capture-by-value vector 024
--FILE--
<?php
$captured = 171;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(53);
--EXPECT--
224
