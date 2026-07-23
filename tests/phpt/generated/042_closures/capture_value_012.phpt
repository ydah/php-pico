--TEST--
closure capture-by-value vector 012
--FILE--
<?php
$captured = 87;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(29);
--EXPECT--
116
