--TEST--
string split transform and padding vector 015
--FILE--
<?php
$parts = explode(',', 'v15,v16,v17');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX15'), ':', str_pad($parts[0], 8, '-');
--EXPECT--
v15|v16|v17:3:V16:mix15:v15-----
