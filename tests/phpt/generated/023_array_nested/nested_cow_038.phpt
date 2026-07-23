--TEST--
nested array copy-on-write vector 038
--FILE--
<?php
$tree = ['node' => ['value' => 153, 'items' => [154]]];
$copy = $tree;
$copy['node']['value'] += 5;
$copy['node']['items'][] = 160;
echo $tree['node']['value'], ':', $copy['node']['value'], ':', implode(',', $tree['node']['items']), ':', implode(',', $copy['node']['items']);
--EXPECT--
153:158:154:154,160
