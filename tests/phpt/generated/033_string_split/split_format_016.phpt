--TEST--
string split transform and padding vector 016
--FILE--
<?php
$parts = explode(',', 'v16,v17,v18');
echo implode('|', $parts), ':', count($parts), ':', strtoupper($parts[1]), ':', strtolower('MIX16'), ':', str_pad($parts[0], 5, '-');
--EXPECT--
v16|v17|v18:3:V17:mix16:v16--
