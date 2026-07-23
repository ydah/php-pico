--TEST--
string split transform and padding vector 026
--FILE--
<?php
$parts = explode(',', 'v26,v27,v28');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX26'), ':', str_pad($parts[0], 7, '-');
--EXPECT--
v26|v27|v28:3:V27:mix26:v26----
