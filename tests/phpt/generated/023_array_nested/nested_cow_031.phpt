--TEST--
nested array copy-on-write vector 031
--FILE--
<?php
$tree = ['node' => ['value' => 125, 'items' => [126]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 132;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
125:130:126:126,132
