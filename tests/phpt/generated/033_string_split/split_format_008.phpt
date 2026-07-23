--TEST--
string split transform and padding vector 008
--FILE--
<?php
$parts = explode(',', 'v8,v9,v10');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX8'), ':', str_pad($parts[0], 4, '-');
--EXPECT--
v8|v9|v10:3:V9:mix8:v8--
