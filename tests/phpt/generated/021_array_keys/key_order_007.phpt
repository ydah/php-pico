--TEST--
associative key order vector 007
--FILE--
<?php
$items = ['left' => 17, 4 => 41, 'right' => 65];
unset($items[4]);
$items['tail'] = 66;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:17,65,66:1
