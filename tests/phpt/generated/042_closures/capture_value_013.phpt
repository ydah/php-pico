--TEST--
closure capture-by-value vector 013
--FILE--
<?php
$captured = 94;
$callback = function ($value) use ($captured) { return $captured + $value; };
$captured = 9999;
echo $callback(31);
--EXPECT--
125
