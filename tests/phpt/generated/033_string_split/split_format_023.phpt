--TEST--
string split transform and padding vector 023
--FILE--
<?php
$parts = explode(',', 'v23,v24,v25');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX23'), ':', str_pad($parts[0], 8, '-');
--EXPECT--
v23|v24|v25:3:V24:mix23:v23-----
