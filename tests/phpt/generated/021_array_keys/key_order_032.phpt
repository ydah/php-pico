--TEST--
associative key order vector 032
--FILE--
<?php
$items = ['left' => 42, 4 => 116, 'right' => 190];
unset($items[4]);
$items['tail'] = 191;
$keys = array_keys($items); $values = array_values($items);
echo implode(',', $keys), ':', implode(',', $values), ':', (array_key_exists('right', $items) ? 1 : 0);
--EXPECT--
left,right,tail:42,190,191:1
