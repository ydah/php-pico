--TEST--
nested array copy-on-write vector 024
--FILE--
<?php
$tree = ['node' => ['value' => 97, 'items' => [98]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 104;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
97:102:98:98,104
