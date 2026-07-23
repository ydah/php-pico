--TEST--
associative key order vector 030
--FILE--
<?php
$items = ['left' => 40, 4 => 110, 'right' => 180];
unset($items[4]);
$items['tail'] = 181;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:40,180,181:1
