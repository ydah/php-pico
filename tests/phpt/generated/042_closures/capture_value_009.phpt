--TEST--
closure capture-by-value vector 009
--FILE--
<?php
$captured = 66;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(23);
--EXPECT--
89
